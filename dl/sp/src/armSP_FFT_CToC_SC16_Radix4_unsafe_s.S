;//
;// 
;// File Name:  armSP_FFT_CToC_SC16_Radix4_unsafe_s.s
;// OpenMAX DL: v1.0.2
;// Last Modified Revision:   7761
;// Last Modified Date:       Wed, 26 Sep 2007
;// 
;// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
;// 
;// 
;//
;// Description:
;// Compute a Radix 4 FFT stage for a N point complex signal
;// 
;// 

        
;// Include standard headers

        INCLUDE omxtypes_s.h
        INCLUDE armCOMM_s.h
        INCLUDE armSP_FFT_s.h
        
        M_VARIANTS CortexA8
        
;// Import symbols required from other files
;// (For example tables)
    
        
        
        
;// Set debugging level        
;//DEBUG_ON    SETL {TRUE}


;// Guarding implementation by the processor name
    

    
    ;// Guarding implementation by the processor name
    
    IF  CortexA8
    
;// Import symbols required from other files
;// (For example tables)
    
    
;//Input Registers

pSrc            RN  0
pDst            RN  2
pTwiddle        RN  1
subFFTNum       RN  6
subFFTSize      RN  7



;//Output Registers


;//Local Scratch Registers

grpCount        RN  3
pointStep       RN  4
outPointStep    RN  5
stepTwiddle     RN  12
setCount        RN  14
srcStep         RN  8
setStep         RN  9
dstStep         RN  10
twStep          RN  11
t1              RN  3

;// Neon Registers

dW1              DN  D0.S16
dW2              DN  D1.S16
dW3              DN  D2.S16   

dXr0             DN  D4.S16
dXi0             DN  D5.S16
dXr1             DN  D6.S16
dXi1             DN  D7.S16
dXr2             DN  D8.S16
dXi2             DN  D9.S16
dXr3             DN  D10.S16
dXi3             DN  D11.S16
dYr0             DN  D12.S16
dYi0             DN  D13.S16
dYr1             DN  D14.S16
dYi1             DN  D15.S16
dYr2             DN  D16.S16
dYi2             DN  D17.S16
dYr3             DN  D18.S16
dYi3             DN  D19.S16
qT0              QN  Q8.S32   
qT1              QN  Q9.S32
qT2              QN  Q6.S32
qT3              QN  Q7.S32

dZr0             DN  D20.S16
dZi0             DN  D21.S16
dZr1             DN  D22.S16
dZi1             DN  D23.S16
dZr2             DN  D24.S16
dZi2             DN  D25.S16
dZr3             DN  D26.S16
dZi3             DN  D27.S16
qY0              QN  Q6.S16
qY1              QN  Q7.S16
qY2              QN  Q8.S16
qY3              QN  Q9.S16   
qX0              QN  Q2.S16
qZ0              QN  Q10.S16
qZ1              QN  Q11.S16
qZ2              QN  Q12.S16
qZ3              QN  Q13.S16

        
        MACRO
        FFTSTAGE $scaled, $inverse , $name
        
        ;// Define stack arguments
        
        
        ;// Update grpCount and grpSize rightaway inorder to reuse pGrpCount and pGrpSize regs
        
        LSL     grpCount,subFFTSize,#2
        LSR     subFFTNum,subFFTNum,#2  
        MOV     subFFTSize,grpCount
        
        
        ;// pOut0+1 increments pOut0 by 4 bytes
        ;// pOut0+outPointStep == increment of 4*outPointStep bytes = size bytes
        
        MOV     stepTwiddle,#0
        SMULBB  outPointStep,grpCount,subFFTNum  
        
        ;// pT0+1 increments pT0 by 4 bytes
        ;// pT0+pointStep = increment of 4*pointStep bytes = grpSize bytes
        
        LSL     pointStep,subFFTNum,#2                      ;// 2*grpSize    
        
        VLD1     dW1,[pTwiddle@64]                             ;//[wi | wr]
        MOV     srcStep,pointStep,LSL #1                    ;// srcStep = 2*pointStep
        VLD1     dW2,[pTwiddle@64]                             ;//[wi | wr]
        ADD     setStep,srcStep,pointStep                   ;// setStep = 3*pointStep
        SUB     srcStep,srcStep,#16                         ;// srcStep = 2*pointStep-16
        VLD1     dW3,[pTwiddle@64]
        ;//RSB     setStep,setStep,#16                      ;// setStep = - 3*pointStep+16
        RSB     setStep,setStep,#0                          ;// setStep = - 3*pointStep
        
        MOV     dstStep,outPointStep,LSL #1
        ADD     dstStep,dstStep,outPointStep                ;// dstStep = 3*outPointStep
        RSB     dstStep,dstStep,#16                         ;// dstStep = - 3*outPointStep+16
        

        
grpLoop$name      
        
        VLD2    {dXr0,dXi0},[pSrc@128],pointStep          ;//  data[0]
        ADD      stepTwiddle,stepTwiddle,pointStep
        VLD2    {dXr1,dXi1},[pSrc@128],pointStep          ;//  data[1]
        ADD      pTwiddle,pTwiddle,stepTwiddle               ;// set pTwiddle to the first point
        VLD2    {dXr2,dXi2},[pSrc@128],pointStep          ;//  data[2]
        MOV      twStep,stepTwiddle,LSL #2
        VLD2    {dXr3,dXi3},[pSrc@128],setStep            ;//  data[3] & reset pSrc 
        
        SUB      twStep,stepTwiddle,twStep                   ;// twStep = -3*stepTwiddle
        
        
        MOV      setCount,pointStep,LSR #2
        ADD     pSrc,pSrc,#16                         ;// set pSrc to data[0] of the next set
        ADD     pSrc,pSrc,pointStep                   ;// increment to data[1] of the next set
       
        ;// Loop on the sets : 4 at a time

setLoop$name        
               
        SUBS    setCount,setCount,#4                    ;// decrement the loop counter
        
        IF  $inverse
            VMULL   qT0,dXr1,dW1[0]
            VMLAL   qT0,dXi1,dW1[1]                       ;// real part
            VMULL   qT1,dXi1,dW1[0]
            VMLSL   qT1,dXr1,dW1[1]                       ;// imag part
            
        ELSE
            VMULL   qT0,dXr1,dW1[0]
            VMLSL   qT0,dXi1,dW1[1]                       ;// real part
            VMULL   qT1,dXi1,dW1[0]
            VMLAL   qT1,dXr1,dW1[1]                       ;// imag part
        
        ENDIF
        
        VLD2    {dXr1,dXi1},[pSrc@128],pointStep          ;//  data[1]
        
        IF  $inverse
            VMULL   qT2,dXr2,dW2[0]
            VMLAL   qT2,dXi2,dW2[1]                       ;// real part
            VMULL   qT3,dXi2,dW2[0]
            VMLSL   qT3,dXr2,dW2[1]                       ;// imag part
            
        ELSE
            VMULL   qT2,dXr2,dW2[0]
            VMLSL   qT2,dXi2,dW2[1]                       ;// real part
            VMULL   qT3,dXi2,dW2[0]
            VMLAL   qT3,dXr2,dW2[1]                       ;// imag part
        
        ENDIF
        
        VRSHRN  dZr1,qT0,#15
        VRSHRN  dZi1,qT1,#15
        
        
        VLD2    {dXr2,dXi2},[pSrc@128],pointStep          ;//  data[2]
        
        IF  $inverse
            VMULL   qT0,dXr3,dW3[0]
            VMLAL   qT0,dXi3,dW3[1]                       ;// real part
            VMULL   qT1,dXi3,dW3[0]
            VMLSL   qT1,dXr3,dW3[1]                       ;// imag part
            
        ELSE
            VMULL   qT0,dXr3,dW3[0]
            VMLSL   qT0,dXi3,dW3[1]                       ;// real part
            VMULL   qT1,dXi3,dW3[0]
            VMLAL   qT1,dXr3,dW3[1]                       ;// imag part
        
        ENDIF
        
        VRSHRN  dZr2,qT2,#15
        VRSHRN  dZi2,qT3,#15
        
        
        VRSHRN  dZr3,qT0,#15
        VRSHRN  dZi3,qT1,#15
        VLD2    {dXr3,dXi3},[pSrc@128],setStep            ;//  data[3] & update pSrc for the next set
        
        
        IF $scaled
        
            ;// finish first stage of 4 point FFT 
            VHADD    qY0,qX0,qZ2
            VHSUB    qY2,qX0,qZ2
                        
            VLD2    {dXr0,dXi0},[pSrc@128]!          ;//  data[0]
            VHADD    qY1,qZ1,qZ3
            VHSUB    qY3,qZ1,qZ3
            
                        
            ;// finish second stage of 4 point FFT 
                                    
            IF  $inverse
                
                VHSUB    qZ0,qY2,qY1
                
                VHADD    dZr2,dYr0,dYi3
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                VHSUB    dZi2,dYi0,dYr3
                
                VHADD    qZ1,qY2,qY1
                VST2    {dZr2,dZi2},[pDst@128],outPointStep
                
                VHSUB    dZr3,dYr0,dYi3
                VST2    {dZr1,dZi1},[pDst@128],outPointStep
                VHADD    dZi3,dYi0,dYr3
                VST2    {dZr3,dZi3},[pDst@128],dstStep
                
                
            ELSE
                
                VHSUB    qZ0,qY2,qY1
                       
                VHSUB    dZr3,dYr0,dYi3
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                VHADD    dZi3,dYi0,dYr3
                        
                VHADD    qZ1,qY2,qY1
                VST2    {dZr3,dZi3},[pDst@128],outPointStep
                        
                VHADD    dZr2,dYr0,dYi3
                VHSUB    dZi2,dYi0,dYr3
                VST2    {dZr1,dZi1},[pDst@128],outPointStep
                VST2    {dZr2,dZi2},[pDst@128],dstStep
                
            
            ENDIF
        
        
        ELSE
        
            ;// finish first stage of 4 point FFT 
            VADD    qY0,qX0,qZ2
            VSUB    qY2,qX0,qZ2
                        
            VLD2    {dXr0,dXi0},[pSrc]!          ;//  data[0]
            VADD    qY1,qZ1,qZ3
            VSUB    qY3,qZ1,qZ3
            
                        
            ;// finish second stage of 4 point FFT 
                                   
                        
            IF  $inverse
                
                VSUB    qZ0,qY2,qY1
                
                VADD    dZr2,dYr0,dYi3
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                VSUB    dZi2,dYi0,dYr3
                
                VADD    qZ1,qY2,qY1
                VST2    {dZr2,dZi2},[pDst@128],outPointStep
                
                VSUB    dZr3,dYr0,dYi3
                VST2    {dZr1,dZi1},[pDst@128],outPointStep
                VADD    dZi3,dYi0,dYr3
                VST2    {dZr3,dZi3},[pDst@128],dstStep
                
                
            ELSE
                
                VSUB    qZ0,qY2,qY1
                       
                VSUB    dZr3,dYr0,dYi3
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                VADD    dZi3,dYi0,dYr3
                        
                VADD    qZ1,qY2,qY1
                VST2    {dZr3,dZi3},[pDst@128],outPointStep
                        
                VADD    dZr2,dYr0,dYi3
                VSUB    dZi2,dYi0,dYr3
                VST2    {dZr1,dZi1},[pDst@128],outPointStep
                VST2    {dZr2,dZi2},[pDst@128],dstStep
                
            
            ENDIF
                                    
                        
            
        ENDIF
        
        ADD     pSrc,pSrc,pointStep                         ;// increment to data[1] of the next set       
        BGT     setLoop$name
        
        VLD1     dW1,[pTwiddle@64],stepTwiddle                 ;//[wi | wr]
        SUBS    grpCount,grpCount,#4                        ;// subtract 4 since grpCount multiplied by 4               
        VLD1     dW2,[pTwiddle@64],stepTwiddle                 ;//[wi | wr]
        ADD     pSrc,pSrc,srcStep                           ;// increment pSrc for the next grp
        VLD1     dW3,[pTwiddle@64],twStep                      ;//[wi | wr]
        
        
        
        BGT     grpLoop$name    

                
        ;// Reset and Swap pSrc and pDst for the next stage
        MOV     t1,pDst
        SUB     pDst,pSrc,outPointStep,LSL #2           ;// pDst -= size; pSrc -= 4*size bytes           
        SUB     pSrc,t1,outPointStep    
        
        
        MEND
        
        
        M_START armSP_FFTFwd_CToC_SC16_Radix4_OutOfPlace_unsafe,r4
            FFTSTAGE {FALSE},{FALSE},FWD
        M_END

        
        M_START armSP_FFTInv_CToC_SC16_Radix4_OutOfPlace_unsafe,r4
            FFTSTAGE {FALSE},{TRUE},INV
        M_END
 
        
        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix4_OutOfPlace_unsafe,r4
            FFTSTAGE {TRUE},{FALSE},FWDSFS
        M_END

        
        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix4_OutOfPlace_unsafe,r4
            FFTSTAGE {TRUE},{TRUE},INVSFS
        M_END

        
    ENDIF                                                           ;//CortexA8
    
 
    
    END