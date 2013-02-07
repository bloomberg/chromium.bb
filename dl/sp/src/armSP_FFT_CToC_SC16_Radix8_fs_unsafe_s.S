;//
;// 
;// File Name:  armSP_FFT_CToC_SC16_Radix8_fs_unsafe_s.s
;// OpenMAX DL: v1.0.2
;// Last Modified Revision:   7766
;// Last Modified Date:       Thu, 27 Sep 2007
;// 
;// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
;// 
;// 
;//
;// Description:
;// Compute a first stage Radix 8 FFT stage for a N point complex signal
;// 
;// 

        
;// Include standard headers

        INCLUDE omxtypes_s.h
        INCLUDE armCOMM_s.h
        
        M_VARIANTS CortexA8
        
;// Import symbols required from other files
;// (For example tables)
    
        
;// Set debugging level        
;//DEBUG_ON    SETL {TRUE}



;// Guarding implementation by the processor name
    
    
    
    
;// Guarding implementation by the processor name
    
    IF  CortexA8
    
;//Input Registers

pSrc            RN  0
pDst            RN  2
pTwiddle        RN  1
subFFTNum       RN  6
subFFTSize      RN  7
pPingPongBuf    RN  5                  ;// dest buffer for the next stage (not pSrc for first stage) 


;//Output Registers


;//Local Scratch Registers

grpSize         RN  3
setCount        RN  3                  ;// Reuse grpSize as setCount
pointStep       RN  4
outPointStep    RN  4
setStep         RN  8
step1           RN  9
step2           RN  10
t0              RN  11
  

;// Neon Registers

dXr0             DN  D14.S16
dXi0             DN  D15.S16
dXr1             DN  D2.S16
dXi1             DN  D3.S16
dXr2             DN  D4.S16
dXi2             DN  D5.S16
dXr3             DN  D6.S16
dXi3             DN  D7.S16
dXr4             DN  D8.S16
dXi4             DN  D9.S16
dXr5             DN  D10.S16
dXi5             DN  D11.S16
dXr6             DN  D12.S16
dXi6             DN  D13.S16
dXr7             DN  D0.S16
dXi7             DN  D1.S16
qX0              QN  Q7.S16
qX1              QN  Q1.S16
qX2              QN  Q2.S16
qX3              QN  Q3.S16   
qX4              QN  Q4.S16
qX5              QN  Q5.S16
qX6              QN  Q6.S16
qX7              QN  Q0.S16

dUr0             DN  D16.S16
dUi0             DN  D17.S16
dUr2             DN  D18.S16
dUi2             DN  D19.S16
dUr4             DN  D20.S16
dUi4             DN  D21.S16
dUr6             DN  D22.S16
dUi6             DN  D23.S16
dUr1             DN  D24.S16
dUi1             DN  D25.S16
dUr3             DN  D26.S16
dUi3             DN  D27.S16
dUr5             DN  D28.S16
dUi5             DN  D29.S16
dUr7             DN  D30.S16                ;// reuse dXr7 and dXi7
dUi7             DN  D31.S16
qU0              QN   Q8.S16
qU1              QN   Q12.S16
qU2              QN   Q9.S16
qU3              QN   Q13.S16   
qU4              QN   Q10.S16
qU5              QN   Q14.S16
qU6              QN   Q11.S16
qU7              QN   Q15.S16



dVr0             DN  D24.S16
dVi0             DN  D25.S16
dVr2             DN  D26.S16
dVi2             DN  D27.S16
dVr4             DN  D28.S16
dVi4             DN  D29.S16
dVr6             DN  D30.S16
dVi6             DN  D31.S16
dVr1             DN  D16.S16
dVi1             DN  D17.S16
dVr3             DN  D18.S16
dVi3             DN  D19.S16
dVr5             DN  D20.S16
dVi5             DN  D21.S16
dVr7             DN  D22.S16              ;// reuse dUi7 
dVi7             DN  D23.S16              ;// reuse dUr7 
qV0              QN  Q12.S16
qV1              QN  Q8.S16
qV2              QN  Q13.S16
qV3              QN  Q9.S16   
qV4              QN  Q14.S16
qV5              QN  Q10.S16
qV6              QN  Q15.S16
qV7              QN  Q11.S16



dYr0             DN  D16.S16
dYi0             DN  D17.S16
dYr2             DN  D18.S16
dYi2             DN  D19.S16
dYr4             DN  D20.S16
dYi4             DN  D21.S16
dYr6             DN  D22.S16
dYi6             DN  D23.S16
dYr1             DN  D24.S16
dYi1             DN  D25.S16
dYr3             DN  D26.S16
dYi3             DN  D27.S16
dYr5             DN  D28.S16
dYi5             DN  D29.S16
dYr7             DN  D30.S16                 ;// reuse dYr4 and dYi4
dYi7             DN  D31.S16
qY0              QN   Q8.S16
qY1              QN   Q12.S16
qY2              QN   Q9.S16
qY3              QN   Q13.S16   
qY4              QN   Q10.S16
qY5              QN   Q14.S16
qY6              QN   Q11.S16
qY7              QN   Q15.S16


dT0              DN  D0.S16             
dT1              DN  D1.S16


;// Define constants
ONEBYSQRT2      EQU   0x00005A82        ;// Q15 format
    

        MACRO
        FFTSTAGE $scaled, $inverse , $name
        
        ;// Define stack arguments
        
        ;// Update pSubFFTSize and pSubFFTNum regs
        MOV     subFFTSize,#8                               ;// subFFTSize = 1 for the first stage
        LDR     t0,=ONEBYSQRT2                              ;// t0=(1/sqrt(2)) as Q15 format
        
        ;// Note: setCount = subFFTNum/8 (reuse the grpSize reg for setCount)
        LSR     grpSize,subFFTNum,#3  
        MOV     subFFTNum,grpSize
        
                
        ;// pT0+1 increments pT0 by 4 bytes
        ;// pT0+pointStep = increment of 4*pointStep bytes = grpSize/2 bytes
        ;// Note: outPointStep = pointStep for firststage
        
        MOV     pointStep,grpSize,LSL #2
        
                                       
        ;// Calculate the step of input data for the next set
        ;//MOV     step1,pointStep,LSL #1                      ;// step1 = 2*pointStep
        VLD2    {dXr0,dXi0},[pSrc@128],pointStep          ;//  data[0]
        MOV     step1,grpSize,LSL #3
        
        MOV     step2,pointStep,LSL #3
        VLD2    {dXr1,dXi1},[pSrc@128],pointStep          ;//  data[1]
        SUB     step2,step2,pointStep                          ;// step2 = 7*pointStep
        RSB     setStep,step2,#16                              ;// setStep = - 7*pointStep+16
        
        
        
        VLD2    {dXr2,dXi2},[pSrc@128],pointStep          ;//  data[2]
        VLD2    {dXr3,dXi3},[pSrc@128],pointStep          ;//  data[3] 
        VLD2    {dXr4,dXi4},[pSrc@128],pointStep          ;//  data[4]
        VLD2    {dXr5,dXi5},[pSrc@128],pointStep          ;//  data[5]
        VLD2    {dXr6,dXi6},[pSrc@128],pointStep          ;//  data[6]
        VLD2    {dXr7,dXi7},[pSrc@128],setStep            ;//  data[7] & update pSrc for the next set
                                                      ;//  setStep = -7*pointStep + 16  
        ;// grp = 0 a special case since all the twiddle factors are 1
        ;// Loop on the sets : 4 sets at a time

grpZeroSetLoop$name
                                                      
        ;// Decrement setcount
        SUBS    setCount,setCount,#4                    ;// decrement the set loop counter           
                                                                         
        
        IF $scaled
            ;// finish first stage of 8 point FFT 
            
            VHADD    qU0,qX0,qX4
            VHADD    qU2,qX1,qX5
            VHADD    qU4,qX2,qX6
            VHADD    qU6,qX3,qX7
            
            ;// finish second stage of 8 point FFT 
            
            VHADD    qV0,qU0,qU4
            VHSUB    qV2,qU0,qU4
            VHADD    qV4,qU2,qU6
            VHSUB    qV6,qU2,qU6
            
            ;// finish third stage of 8 point FFT 
            
            VHADD    qY0,qV0,qV4
            VHSUB    qY4,qV0,qV4
            VST2    {dYr0,dYi0},[pDst@128],step1                    ;// store y0
            
            IF  $inverse
                
                VHSUB    dYr2,dVr2,dVi6
                VHADD    dYi2,dVi2,dVr6
                
                VHADD    dYr6,dVr2,dVi6
                VST2    {dYr2,dYi2},[pDst@128],step1                    ;// store y2
                VHSUB    dYi6,dVi2,dVr6
            
                VHSUB    qU1,qX0,qX4                    
                VST2    {dYr4,dYi4},[pDst@128],step1                    ;// store y4
            
                VHSUB    qU3,qX1,qX5
                VHSUB    qU5,qX2,qX6
                VST2    {dYr6,dYi6},[pDst@128],step1                    ;// store y6
            
            ELSE
            
                VHADD    dYr6,dVr2,dVi6
                VHSUB    dYi6,dVi2,dVr6
                
                VHSUB    dYr2,dVr2,dVi6
                VST2    {dYr6,dYi6},[pDst@128],step1                    ;// store y2
                VHADD    dYi2,dVi2,dVr6
                
                                
                VHSUB    qU1,qX0,qX4
                VST2    {dYr4,dYi4},[pDst@128],step1                    ;// store y4
                VHSUB    qU3,qX1,qX5
                VHSUB    qU5,qX2,qX6
                VST2    {dYr2,dYi2},[pDst@128],step1                    ;// store y6

            
            ENDIF
            
            ;// finish first stage of 8 point FFT 
            
            VHSUB    qU7,qX3,qX7
            VMOV    dT0[0],t0                                   
            
            ;// finish second stage of 8 point FFT 
            
            VHSUB    dVr1,dUr1,dUi5
            VLD2    {dXr0,dXi0},[pSrc@128],pointStep          ;//  data[0] for next iteration
            VHADD    dVi1,dUi1,dUr5
            VHADD    dVr3,dUr1,dUi5
            VLD2    {dXr1,dXi1},[pSrc@128],pointStep          ;//  data[1]
            VHSUB    dVi3,dUi1,dUr5
                        
            VHSUB    dVr5,dUr3,dUi7
            VLD2    {dXr2,dXi2},[pSrc@128],pointStep          ;//  data[2]
            VHADD    dVi5,dUi3,dUr7
            VHADD    dVr7,dUr3,dUi7
            VLD2    {dXr3,dXi3},[pSrc@128],pointStep          ;//  data[3]
            VHSUB    dVi7,dUi3,dUr7
            
            ;// finish third stage of 8 point FFT 
            
            IF  $inverse
            
                ;// calculate a*v5 
                VQRDMULH    dT1,dVr5,dT0[0]                         ;// use dVi0 for dT1
                VLD2    {dXr4,dXi4},[pSrc@128],pointStep          ;//  data[4]
                VQRDMULH    dVi5,dVi5,dT0[0]
                            
                VLD2    {dXr5,dXi5},[pSrc@128],pointStep          ;//  data[5]
                VSUB    dVr5,dT1,dVi5                               ;// a * V5
                VADD    dVi5,dT1,dVi5
                
                VLD2    {dXr6,dXi6},[pSrc@128],pointStep          ;//  data[6]
                
                ;// calculate  b*v7
                VQRDMULH    dT1,dVr7,dT0[0]
                VQRDMULH    dVi7,dVi7,dT0[0]
                
                VHADD    qY1,qV1,qV5
                VHSUB    qY5,qV1,qV5
                
                            
                VADD    dVr7,dT1,dVi7                               ;// b * V7
                VSUB    dVi7,dVi7,dT1
                SUB     pDst, pDst, step2                           ;// set pDst to y1
                
                VLD2    {dXr7,dXi7},[pSrc@128],setStep            ;//  data[7]            
                
                
                VHSUB    dYr3,dVr3,dVr7
                VHSUB    dYi3,dVi3,dVi7
                VST2    {dYr1,dYi1},[pDst@128],step1                    ;// store y1
                VHADD    dYr7,dVr3,dVr7
                VHADD    dYi7,dVi3,dVi7

                
                VST2    {dYr3,dYi3},[pDst@128],step1                    ;// store y3
                VST2    {dYr5,dYi5},[pDst@128],step1                    ;// store y5
                VST2    {dYr7,dYi7},[pDst@128],#16                      ;// store y7
            ELSE
            
                ;// calculate  b*v7
                VQRDMULH    dT1,dVr7,dT0[0]
                VLD2    {dXr4,dXi4},[pSrc@128],pointStep          ;//  data[4]
                VQRDMULH    dVi7,dVi7,dT0[0]
                
                VLD2    {dXr5,dXi5},[pSrc@128],pointStep          ;//  data[5]
                VADD    dVr7,dT1,dVi7                               ;// b * V7
                VSUB    dVi7,dVi7,dT1
                
                VLD2    {dXr6,dXi6},[pSrc@128],pointStep          ;//  data[6]
                
                ;// calculate a*v5 
                VQRDMULH    dT1,dVr5,dT0[0]                         ;// use dVi0 for dT1
                VQRDMULH    dVi5,dVi5,dT0[0]

                VHADD    dYr7,dVr3,dVr7
                VHADD    dYi7,dVi3,dVi7
                SUB     pDst, pDst, step2                           ;// set pDst to y1
            
                VSUB    dVr5,dT1,dVi5                               ;// a * V5
                VADD    dVi5,dT1,dVi5
                VLD2    {dXr7,dXi7},[pSrc@128],setStep            ;//  data[7]            
                
                VHSUB    qY5,qV1,qV5
                
                VHSUB    dYr3,dVr3,dVr7
                VST2    {dYr7,dYi7},[pDst@128],step1                    ;// store y1
                VHSUB    dYi3,dVi3,dVi7
                VHADD    qY1,qV1,qV5
                
                
                VST2    {dYr5,dYi5},[pDst@128],step1                    ;// store y3
                VST2    {dYr3,dYi3},[pDst@128],step1                    ;// store y5
                VST2    {dYr1,dYi1},[pDst@128],#16                      ;// store y7

            
            ENDIF
            
            
           
        ELSE
            ;// finish first stage of 8 point FFT 
            
            VADD    qU0,qX0,qX4
            VADD    qU2,qX1,qX5
            VADD    qU4,qX2,qX6
            VADD    qU6,qX3,qX7
            
            ;// finish second stage of 8 point FFT 
            
            VADD    qV0,qU0,qU4
            VSUB    qV2,qU0,qU4
            VADD    qV4,qU2,qU6
            VSUB    qV6,qU2,qU6
            
            ;// finish third stage of 8 point FFT 
            
            VADD    qY0,qV0,qV4
            VSUB    qY4,qV0,qV4
            VST2    {dYr0,dYi0},[pDst@128],step1                    ;// store y0
            
            IF  $inverse
                
                VSUB    dYr2,dVr2,dVi6
                VADD    dYi2,dVi2,dVr6
                
                VADD    dYr6,dVr2,dVi6
                VST2    {dYr2,dYi2},[pDst@128],step1                    ;// store y2
                VSUB    dYi6,dVi2,dVr6
            
                VSUB    qU1,qX0,qX4                    
                VST2    {dYr4,dYi4},[pDst@128],step1                    ;// store y4
            
                VSUB    qU3,qX1,qX5
                VSUB    qU5,qX2,qX6
                VST2    {dYr6,dYi6},[pDst@128],step1                    ;// store y6
            
            ELSE
            
                VADD    dYr6,dVr2,dVi6
                VSUB    dYi6,dVi2,dVr6
                
                VSUB    dYr2,dVr2,dVi6
                VST2    {dYr6,dYi6},[pDst@128],step1                    ;// store y2
                VADD    dYi2,dVi2,dVr6
                
                                
                VSUB    qU1,qX0,qX4
                VST2    {dYr4,dYi4},[pDst@128],step1                    ;// store y4
                VSUB    qU3,qX1,qX5
                VSUB    qU5,qX2,qX6
                VST2    {dYr2,dYi2},[pDst@128],step1                    ;// store y6

            
            ENDIF
            
            ;// finish first stage of 8 point FFT 
            
            VSUB    qU7,qX3,qX7
            VMOV    dT0[0],t0                                   
            
            ;// finish second stage of 8 point FFT 
            
            VSUB    dVr1,dUr1,dUi5
            VLD2    {dXr0,dXi0},[pSrc@128],pointStep          ;//  data[0] for next iteration
            VADD    dVi1,dUi1,dUr5
            VADD    dVr3,dUr1,dUi5
            VLD2    {dXr1,dXi1},[pSrc@128],pointStep          ;//  data[1]
            VSUB    dVi3,dUi1,dUr5
                        
            VSUB    dVr5,dUr3,dUi7
            VLD2    {dXr2,dXi2},[pSrc@128],pointStep          ;//  data[2]
            VADD    dVi5,dUi3,dUr7
            VADD    dVr7,dUr3,dUi7
            VLD2    {dXr3,dXi3},[pSrc@128],pointStep          ;//  data[3]
            VSUB    dVi7,dUi3,dUr7
            
            ;// finish third stage of 8 point FFT 
            
            IF  $inverse
            
                ;// calculate a*v5 
                VQRDMULH    dT1,dVr5,dT0[0]                         ;// use dVi0 for dT1
                VLD2    {dXr4,dXi4},[pSrc@128],pointStep          ;//  data[4]
                VQRDMULH    dVi5,dVi5,dT0[0]
                            
                VLD2    {dXr5,dXi5},[pSrc@128],pointStep          ;//  data[5]
                VSUB    dVr5,dT1,dVi5                               ;// a * V5
                VADD    dVi5,dT1,dVi5
                
                VLD2    {dXr6,dXi6},[pSrc@128],pointStep          ;//  data[6]
                
                ;// calculate  b*v7
                VQRDMULH    dT1,dVr7,dT0[0]
                VQRDMULH    dVi7,dVi7,dT0[0]
                
                VADD    qY1,qV1,qV5
                VSUB    qY5,qV1,qV5
                
                            
                VADD    dVr7,dT1,dVi7                               ;// b * V7
                VSUB    dVi7,dVi7,dT1
                SUB     pDst, pDst, step2                           ;// set pDst to y1
                
                VLD2    {dXr7,dXi7},[pSrc@128],setStep            ;//  data[7]            
                
                
                VSUB    dYr3,dVr3,dVr7
                VSUB    dYi3,dVi3,dVi7
                VST2    {dYr1,dYi1},[pDst@128],step1                    ;// store y1
                VADD    dYr7,dVr3,dVr7
                VADD    dYi7,dVi3,dVi7

                
                VST2    {dYr3,dYi3},[pDst@128],step1                    ;// store y3
                VST2    {dYr5,dYi5},[pDst@128],step1                    ;// store y5
                VST2    {dYr7,dYi7},[pDst@128],#16                      ;// store y7
            ELSE
            
                ;// calculate  b*v7
                VQRDMULH    dT1,dVr7,dT0[0]
                VLD2    {dXr4,dXi4},[pSrc@128],pointStep          ;//  data[4]
                VQRDMULH    dVi7,dVi7,dT0[0]
                
                VLD2    {dXr5,dXi5},[pSrc@128],pointStep          ;//  data[5]
                VADD    dVr7,dT1,dVi7                               ;// b * V7
                VSUB    dVi7,dVi7,dT1
                
                VLD2    {dXr6,dXi6},[pSrc@128],pointStep          ;//  data[6]
                
                ;// calculate a*v5 
                VQRDMULH    dT1,dVr5,dT0[0]                         ;// use dVi0 for dT1
                VQRDMULH    dVi5,dVi5,dT0[0]

                VADD    dYr7,dVr3,dVr7
                VADD    dYi7,dVi3,dVi7
                SUB     pDst, pDst, step2                           ;// set pDst to y1
            
                VSUB    dVr5,dT1,dVi5                               ;// a * V5
                VADD    dVi5,dT1,dVi5
                VLD2    {dXr7,dXi7},[pSrc@128],setStep            ;//  data[7]            
                
                VSUB    qY5,qV1,qV5
                
                VSUB    dYr3,dVr3,dVr7
                VST2    {dYr7,dYi7},[pDst@128],step1                    ;// store y1
                VSUB    dYi3,dVi3,dVi7
                VADD    qY1,qV1,qV5
                
                
                VST2    {dYr5,dYi5},[pDst@128],step1                    ;// store y3
                VST2    {dYr3,dYi3},[pDst@128],step1                    ;// store y5
                VST2    {dYr1,dYi1},[pDst@128],#16                      ;// store y7

            
            ENDIF
            
            
        ENDIF
        
        SUB     pDst, pDst, step2                               ;// update pDst for the next set
        BGT     grpZeroSetLoop$name
        
        
        ;// reset pSrc to pDst for the next stage
        SUB     pSrc,pDst,pointStep                             ;// pDst -= 2*grpSize  
        MOV     pDst,pPingPongBuf 
        
        
        
        MEND
        

        ;// Allocate stack memory required by the function
        
        
        M_START armSP_FFTFwd_CToC_SC16_Radix8_fs_OutOfPlace_unsafe,r4
            FFTSTAGE {FALSE},{FALSE},FWD
        M_END

        
        M_START armSP_FFTInv_CToC_SC16_Radix8_fs_OutOfPlace_unsafe,r4
            FFTSTAGE {FALSE},{TRUE},INV
        M_END
 
        
        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix8_fs_OutOfPlace_unsafe,r4
            FFTSTAGE {TRUE},{FALSE},FWDSFS
        M_END

        
        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix8_fs_OutOfPlace_unsafe,r4
            FFTSTAGE {TRUE},{TRUE},INVSFS
        M_END

    
    ENDIF                                                           ;//CortexA8
        
    
     
    END